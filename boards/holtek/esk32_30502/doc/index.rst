.. _esk32_30502:

ESK32-30502
###########

Overview
********

The ESK32-30502 is a starter kit from Holtek featuring the HT32F52341
microcontroller. The board is designed for evaluation and development
of applications using the HT32F52341 ARM Cortex-M0+ MCU.

Hardware
********

The HT32F52341 features:

- ARM Cortex-M0+ processor running at up to 48 MHz
- 64 KB Flash memory
- 8 KB SRAM
- USB 2.0 Full-speed device interface
- Multiple communication interfaces: USART, UART, SPI, I2C
- 12-bit ADC
- Multiple timers (MCTM, GPTM, SCTM, BFTM)
- RTC with calendar
- Watchdog timer
- Up to 38 GPIO pins

Supported Features
==================

The esk32_30502 board configuration supports the following hardware features:

+-----------+------------+-------------------------------------+
| Interface | Controller | Driver/Component                    |
+===========+============+=====================================+
| NVIC      | on-chip    | nested vector interrupt controller  |
+-----------+------------+-------------------------------------+
| SYSTICK   | on-chip    | systick                             |
+-----------+------------+-------------------------------------+
| UART      | on-chip    | serial port                         |
+-----------+------------+-------------------------------------+
| GPIO      | on-chip    | gpio                                |
+-----------+------------+-------------------------------------+

Other hardware features are not yet supported by this Zephyr port.

The default configuration can be found in the defconfig file:
:zephyr_file:`boards/holtek/esk32_30502/esk32_30502_defconfig`

Connections and IOs
===================

The ESK32-30502 board has 3 user LEDs:

- LED1 (PB4)
- LED2 (PA14)
- LED3 (PA15)

Programming and Debugging
*************************

The ESK32-30502 includes an onboard e-Link32 Pro debugger which provides:

- Programming and debugging via SWD
- Virtual COM port for UART communication

Flashing
========

The board can be flashed using the onboard e-Link32 Pro debugger or an
external debugger connected to the SWD connector.

To flash the board using OpenOCD:

.. code-block:: console

   west flash

Debugging
=========

To debug the board using OpenOCD:

.. code-block:: console

   west debug

References
**********

.. target-notes::

.. _Holtek HT32F52341 Website:
   https://www.holtek.com/productdetail/-/vg/52341

.. _ESK32-30502 Website:
   https://www.holtek.com/esk32-30502
