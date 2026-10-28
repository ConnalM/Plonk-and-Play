# P&P serial acceptance tooling rule

During physical acceptance, the P&P CH340 COM port is owned exclusively by
one dedicated direct serial-capture process.

The ChatGPT/Codex computer-use helpers (`codex-computer-use.exe` and
`codex-computer-use-swift.exe`) must not open, probe, test or otherwise acquire
that COM port. They can be used for unrelated PC interaction, but never as the
serial-port owner during an acceptance run.

Before a run, identify the current CH340 port, perform any open/close checks,
then start exactly one capture. Once capture starts, do not run competing
serial probes or monitors. Treat a later Windows `Access Denied` on the P&P
COM port as a tooling-ownership issue first, not as evidence of a P&P
firmware or hardware failure.
