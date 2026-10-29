#include <cstdlib>
#include <iostream>

// Deterministic host model of the production owner-service policy. The
// physical owner and controller loop use the same state transitions: boot gets
// one normal mount attempt, mount success is provisional until reconstruction
// commits health, and every failed attempt latches until a new explicit request.
enum class Life { Ready, Finished, Racing };
enum class Request { Accepted, Pending, Healthy, Rejected };

class RecoveryPolicy {
public:
  Request request(Life life) {
    if (!eligible(life)) return Request::Rejected;
    if (durable_) return Request::Healthy;
    if (pending_ || mounted_) return Request::Pending;
    pending_ = true;
    return Request::Accepted;
  }

  bool boot(bool mountOk, bool historyOk, bool detailOk, bool diagnosticsOk) {
    if (mounted_ || attempted_) return false;
    attempted_ = true;
    ++mountAttempts_;
    if (!mountOk) {
      failureLatched_ = true;
      explicitRecoveryRequired_ = true;
      return true;
    }
    mounted_ = true;
    durable_ = false;
    if (historyOk && detailOk && diagnosticsOk) {
      durable_ = true;
      return true;
    }
    mounted_ = false;
    failureLatched_ = true;
    explicitRecoveryRequired_ = true;
    return true;
  }

  bool service(Life life, bool mountOk, bool historyOk, bool detailOk, bool diagnosticsOk) {
    if (!eligible(life) || mounted_) return false;
    const bool explicitRequest = pending_;
    if (!explicitRequest) return false;
    pending_ = false;
    attempted_ = true;
    ++mountAttempts_;
    ++teardownCount_;
    if (!mountOk) { failureLatched_ = true; explicitRecoveryRequired_ = true; return true; }
    mounted_ = true;
    durable_ = false;
    if (historyOk && detailOk && diagnosticsOk) {
      durable_ = true;
      failureLatched_ = false;
      explicitRecoveryRequired_ = false;
    } else {
      mounted_ = false;
      failureLatched_ = true;
      explicitRecoveryRequired_ = true;
    }
    return true;
  }

  void lifecycleChanged() { attempted_ = false; }
  void physicalIoFailure() {
    mounted_ = false;
    durable_ = false;
    failureLatched_ = true;
    explicitRecoveryRequired_ = true;
  }
  bool mounted() const { return mounted_; }
  bool durable() const { return durable_; }
  bool failureLatched() const { return failureLatched_; }
  bool explicitRecoveryRequired() const { return explicitRecoveryRequired_; }
  unsigned mountAttempts() const { return mountAttempts_; }
  unsigned teardownCount() const { return teardownCount_; }

private:
  static bool eligible(Life life) { return life == Life::Ready || life == Life::Finished; }
  bool mounted_ = false;
  bool durable_ = false;
  bool pending_ = false;
  bool attempted_ = false;
  bool failureLatched_ = false;
  bool explicitRecoveryRequired_ = false;
  unsigned mountAttempts_ = 0;
  unsigned teardownCount_ = 0;
};

static void check(bool value, const char* name) {
  if (!value) { std::cerr << "FAIL: " << name << "\n"; std::exit(1); }
  std::cout << "PASS: " << name << "\n";
}

int main() {
  RecoveryPolicy bootAbsent;
  check(bootAbsent.boot(false, false, false, false), "absent-SD boot attempts one normal mount");
  check(bootAbsent.mountAttempts() == 1 && bootAbsent.explicitRecoveryRequired() &&
        !bootAbsent.mounted() && !bootAbsent.durable(),
        "failed boot mount latches unavailable and explicit recovery");
  check(!bootAbsent.service(Life::Ready, true, true, true, true),
        "controller service performs no automatic recovery after boot failure");
  check(bootAbsent.mountAttempts() == 1, "repeated service performs zero additional boot-failure mounts");
  bootAbsent.lifecycleChanged();
  check(!bootAbsent.service(Life::Finished, true, true, true, true) &&
        bootAbsent.mountAttempts() == 1,
        "lifecycle change and reinsertion-equivalent state remain inert");
  check(bootAbsent.request(Life::Finished) == Request::Accepted,
        "authorised retry permits recovery after failed boot");
  check(bootAbsent.service(Life::Finished, true, true, true, true) &&
        bootAbsent.mountAttempts() == 2,
        "authorised retry performs exactly one later mount attempt");
  check(bootAbsent.durable(), "successful retry restores durable health");

  RecoveryPolicy startup;
  check(startup.boot(true, true, true, true), "SD-present boot attempts one normal mount");
  check(startup.mounted() && startup.durable(), "successful boot reconstruction commits durable health");

  RecoveryPolicy checkFailure;
  check(checkFailure.boot(true, true, true, true), "check-failure fixture has a mounted baseline");
  checkFailure.physicalIoFailure();
  check(checkFailure.explicitRecoveryRequired() && !checkFailure.mounted() && !checkFailure.durable(),
        "runtime physical I/O failure latches unavailable and explicit recovery");
  const unsigned beforeService = checkFailure.mountAttempts();
  check(!checkFailure.service(Life::Ready, true, true, true, true),
        "controller loop performs no automatic recovery after Check storage failure");
  check(checkFailure.mountAttempts() == beforeService, "repeated service performs zero mount attempts after failure");
  checkFailure.lifecycleChanged();
  check(!checkFailure.service(Life::Finished, true, true, true, true),
        "lifecycle change and reinsertion-equivalent state do not recover automatically");
  check(checkFailure.request(Life::Finished) == Request::Accepted, "authorised retry permits recovery after failure");
  check(checkFailure.service(Life::Finished, true, true, true, true), "authorised retry performs one recovery attempt");
  check(checkFailure.mountAttempts() == beforeService + 1 && checkFailure.teardownCount() == 1,
        "retry tears down stale mount state before the genuine retry attempt");
  check(checkFailure.durable(), "successful retry restores durable health");

  RecoveryPolicy mountOnly;
  check(mountOnly.boot(true, false, true, true), "mount-only boot attempt runs");
  check(!mountOnly.durable() && mountOnly.failureLatched(), "mount success alone is not durable and reconstruction failure latches");
  mountOnly.lifecycleChanged();
  check(!mountOnly.service(Life::Finished, true, true, true, true), "lifecycle change cannot retry failed automatic recovery");
  check(mountOnly.request(Life::Finished) == Request::Accepted, "new explicit request clears failed-attempt gate");
  check(mountOnly.service(Life::Finished, true, true, true, true), "explicit retry performs exactly one attempt");
  check(mountOnly.durable(), "explicit retry restores durable health after reconstruction");

  RecoveryPolicy explicitFailure;
  check(explicitFailure.request(Life::Ready) == Request::Accepted, "explicit request accepted");
  check(explicitFailure.request(Life::Ready) == Request::Pending, "duplicate request is bounded while pending");
  check(explicitFailure.service(Life::Ready, false, false, false, false), "failed explicit request attempts once");
  explicitFailure.lifecycleChanged();
  check(!explicitFailure.service(Life::Finished, true, true, true, true), "failed explicit recovery cannot auto-retry");
  check(explicitFailure.request(Life::Finished) == Request::Accepted, "second explicit request is required");
  check(explicitFailure.service(Life::Finished, true, true, true, true), "second explicit request attempts once");
  check(explicitFailure.durable(), "second explicit request commits healthy state");

  RecoveryPolicy failedRetry;
  check(failedRetry.request(Life::Ready) == Request::Accepted, "failed-retry request accepted once");
  check(failedRetry.service(Life::Ready, false, false, false, false), "failed retry performs one mount attempt");
  check(failedRetry.explicitRecoveryRequired(), "failed retry remains explicitly latched");
  failedRetry.lifecycleChanged();
  check(!failedRetry.service(Life::Finished, true, true, true, true), "failed retry cannot auto-retry after lifecycle change");
  check(failedRetry.request(Life::Finished) == Request::Accepted, "failed retry requires a new request");

  RecoveryPolicy prohibited;
  check(prohibited.request(Life::Racing) == Request::Rejected, "active race rejects recovery request");
  check(!prohibited.service(Life::Racing, true, true, true, true), "active race performs no mount attempt");
  std::cout << "Retry storage recovery policy PASS\n";
}
