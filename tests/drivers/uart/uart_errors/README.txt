The purpose of this test is to validate UART receiver when error occurs on
the line. Error is generated by the second UART driver instance which sends
certain bytes with parity enabled when receiver is configured without parity.
Additional bit triggers framing error.
