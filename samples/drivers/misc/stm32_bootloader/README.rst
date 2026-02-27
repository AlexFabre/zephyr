.. zephyr:code-sample:: stm32_bootloader
   :name: STM32 System Bootloader
   :relevant-api: stm32_bootloader

   Program a target STM32 via its system bootloader.

Overview
********

STM32 devices have an internal bootloader stored in the internal boot ROM
(system memory) programmed during production, that aims to let users download
application program to the internal flash memory through one of the available
serial peripherals (such as USART, CAN, USB, I2C, I3C, SPI, FDCAN).

The list of the bootloader active peripherals for each STM32 reference,
is accessible in `AN2606`_.

This sample demonstrates how to connect to a target STM32, and read/write it's
flash memory via UART using the `AN3155`_ protocol.

Requirements
************

To use this sample, the following hardware is required:

* Any host board with serial output and gpio to control
  the target STM32 reset pin and BOOT pin(s).
* A target STM32 board with a bootloader compatible serial
  interface, and reset pin (NRST) and BOOT pin(s) accessible.

The BOOT pin(s) sequence to enter the system bootloader may differ a little
depending on the STM32 reference, also some STM32 may have their bootloader
entry by BOOT pin(s) disabled if their flash memory was previously written.
Its therefore strongly recommended to read the exact requirements for your
target STM32 listed in `AN2606`_.

Wiring
******

On STM32 side, the UART interface has to be connected to one of
the UART activated with the system bootloader. The list of the 
bootloader active peripherals for each STM32 ref, and its pin mapping
is accessible in `AN2606`_.

This is an example mapping with a stm32h573i_dk host, connected to a
target nucleo_g0b1re on its UART1.

+---------------+----------------+
| stm32h573i_dk | nucleo_g0b1re  |
+===============+================+
| PB10 /        | PA10 /         |
| (USART3_TX)   | (USART1_RX)    |
+---------------+----------------+
| PB11 /        | PA9 /          |
| (USART3_RX).  | (USART1_TX)    |
+---------------+----------------+
| PG8 /         | PA14 /         |
| (IO)          | (BOOT0)        |
+---------------+----------------+
| PG5 /         | NRST           |
| (IO)          |                |
+---------------+----------------+

Building and Running
********************

This sample should work on any board that has GPIO and SERIAL enabled.
For example, it can be run on the stm32h573i_dk board as
described below:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/misc/stm32_bootloader
   :board: stm32h573i_dk
   :goals: flash
   :compact:

.. _AN2606: https://www.st.com/resource/en/application_note/cd00167594-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf
.. _AN3155: https://www.st.com/resource/en/application_note/an3155-how-to-use-usart-protocol-in-bootloader-on-stm32-mcus-stmicroelectronics.pdf
