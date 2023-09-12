################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/CDCHandler.cpp \
../src/GyroSPI.cpp \
../src/USB.cpp \
../src/USBHandler.cpp \
../src/app_ble.cpp \
../src/app_entry.cpp \
../src/bas_app.cpp \
../src/dis_app.cpp \
../src/hids_app.cpp \
../src/initialisation.cpp \
../src/interrupts.cpp \
../src/main.cpp 

OBJS += \
./src/CDCHandler.o \
./src/GyroSPI.o \
./src/USB.o \
./src/USBHandler.o \
./src/app_ble.o \
./src/app_entry.o \
./src/bas_app.o \
./src/dis_app.o \
./src/hids_app.o \
./src/initialisation.o \
./src/interrupts.o \
./src/main.o 

CPP_DEPS += \
./src/CDCHandler.d \
./src/GyroSPI.d \
./src/USB.d \
./src/USBHandler.d \
./src/app_ble.d \
./src/app_entry.d \
./src/bas_app.d \
./src/dis_app.d \
./src/hids_app.d \
./src/initialisation.d \
./src/interrupts.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o src/%.su src/%.cyclo: ../src/%.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++17 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32WB35xx -c -I../Drivers/STM32WBxx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32WBxx/Include -I../Drivers/CMSIS/Include -I"D:/Eurorack/Ball/Ball_Base_Test/STM_Files" -I"D:/Eurorack/Ball/Ball_Base_Test/src" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/utilities" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/template" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Inc" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/auto" -I"D:/Eurorack/Ball/Ball_Base_Test/STM32_WPAN/App" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/lpm/tiny_lpm" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/sequencer" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Src" -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-src

clean-src:
	-$(RM) ./src/CDCHandler.cyclo ./src/CDCHandler.d ./src/CDCHandler.o ./src/CDCHandler.su ./src/GyroSPI.cyclo ./src/GyroSPI.d ./src/GyroSPI.o ./src/GyroSPI.su ./src/USB.cyclo ./src/USB.d ./src/USB.o ./src/USB.su ./src/USBHandler.cyclo ./src/USBHandler.d ./src/USBHandler.o ./src/USBHandler.su ./src/app_ble.cyclo ./src/app_ble.d ./src/app_ble.o ./src/app_ble.su ./src/app_entry.cyclo ./src/app_entry.d ./src/app_entry.o ./src/app_entry.su ./src/bas_app.cyclo ./src/bas_app.d ./src/bas_app.o ./src/bas_app.su ./src/dis_app.cyclo ./src/dis_app.d ./src/dis_app.o ./src/dis_app.su ./src/hids_app.cyclo ./src/hids_app.d ./src/hids_app.o ./src/hids_app.su ./src/initialisation.cyclo ./src/initialisation.d ./src/initialisation.o ./src/initialisation.su ./src/interrupts.cyclo ./src/interrupts.d ./src/interrupts.o ./src/interrupts.su ./src/main.cyclo ./src/main.d ./src/main.o ./src/main.su

.PHONY: clean-src

