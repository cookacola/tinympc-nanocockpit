# Exercises

1. Run the minimal dry-run command and identify which steps require hardware.
2. Change `host` in `tutorial_impl/configs/minimal.json` to `192.168.4.1` and confirm the stream verification command changes.
3. Enable `NETWORK_TEST_INPUT` in `src/gap/examples/pulp-frontnet/config.h` and run the documented GVSOC command.
4. Read `src/client/aideck_cpx_streamer/aideck_cpx_streamer/cpx/streamer.py::send_reply` and compare it to `tutorial_impl/actions_or_control/reply.py`.
5. Add a test that validates the selected `NETWORK_NAME` in `src/gap/examples/pulp-frontnet/app/app.mk`.
