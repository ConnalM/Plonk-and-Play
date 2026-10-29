#include <cstdlib>
#include <iostream>

// Deterministic host model of the production owner-service policy. The
// physical owner and controller loop use the same state transitions: mount
// success is provisional, reconstruction commits health, and every failed
// attempt latches until a new explicit request.
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

  bool service(Life life, bool mountOk, bool historyOk, bool detailOk, bool diagnosticsOk) {
    if (!eligible(life) || mounted_) return false;
    const bool explicitRequest = pending_;
    const bool automaticRequest = !failureLatched_ && !attempted_ && !explicitRecoveryRequired_;
    if (!explicitRequest && !automaticRequest) return false;
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
  RecoveryPolicy startup;
  check(startup.service(Life::Ready, true, true, true, true), "initial automatic mount attempted once");
  check(startup.mounted() && startup.durable(), "mount plus reconstruction commits durable health");

  RecoveryPolicy checkFailure;
  check(checkFailure.service(Life::Ready, true, true, true, true), "check-failure fixture has a mounted baseline");
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
  check(checkFailure.mountAttempts() == beforeService + 1 && checkFailure.teardownCount() == checkFailure.mountAttempts(),
        "retry tears down stale mount state before each genuine attempt");
  check(checkFailure.durable(), "successful retry restores durable health");

  RecoveryPolicy mountOnly;
  check(mountOnly.service(Life::Ready, true, false, true, true), "mount-only recovery attempt runs");
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
