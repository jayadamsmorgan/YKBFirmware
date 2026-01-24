cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

target_sources(app PRIVATE
  src/main.c
  src/app_esb.c
  src/esb.c
  src/hci_rpmsg_module.c
  src/timeslot_handler.c
)
