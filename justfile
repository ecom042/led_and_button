alias b := build
alias c := clean
alias f := flash
alias r := run
alias m := monitor
alias t := test

default:
    just --list

clean:
    rip build

build:
    west build -p -b qemu_riscv32 .

test:
    west twister -p qemu_riscv32 -T tests

run_button_tests: && run
    west build -p -b qemu_riscv32 ./tests/button

flash:
    west flash

run:
    west build -t run

monitor:
    tio -b 115200 /dev/tty.usbmodem0006831335301
