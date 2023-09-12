################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.c \
../Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.c \
../Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.c \
../Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.c \
../Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.c 

C_DEPS += \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.d \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.d \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.d \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.d \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.d 

OBJS += \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.o \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.o \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.o \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.o \
./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/ST/STM32_WPAN/ble/core/auto/%.o Middlewares/ST/STM32_WPAN/ble/core/auto/%.su Middlewares/ST/STM32_WPAN/ble/core/auto/%.cyclo: ../Middlewares/ST/STM32_WPAN/ble/core/auto/%.c Middlewares/ST/STM32_WPAN/ble/core/auto/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32WB35xx -DUSE_HAL_DRIVER -c -I../Drivers/STM32WBxx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32WBxx/Include -I../Drivers/CMSIS/Include -I"D:/Eurorack/Ball/Ball_Base_Test/STM_Files" -I"D:/Eurorack/Ball/Ball_Base_Test/src" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/utilities" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/template" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Inc" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/auto" -I"D:/Eurorack/Ball/Ball_Base_Test/STM32_WPAN/App" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/lpm/tiny_lpm" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/sequencer" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Src" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-ST-2f-STM32_WPAN-2f-ble-2f-core-2f-auto

clean-Middlewares-2f-ST-2f-STM32_WPAN-2f-ble-2f-core-2f-auto:
	-$(RM) ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.cyclo ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.d ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.o ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.su ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.cyclo ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.d ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.o ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.su ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.cyclo ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.d ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.o ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.su ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.cyclo ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.d ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.o ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.su ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.cyclo ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.d ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.o ./Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.su

.PHONY: clean-Middlewares-2f-ST-2f-STM32_WPAN-2f-ble-2f-core-2f-auto

