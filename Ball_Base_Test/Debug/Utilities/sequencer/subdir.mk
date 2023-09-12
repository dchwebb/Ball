################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Utilities/sequencer/stm32_seq.c 

C_DEPS += \
./Utilities/sequencer/stm32_seq.d 

OBJS += \
./Utilities/sequencer/stm32_seq.o 


# Each subdirectory must supply rules for building sources it contributes
Utilities/sequencer/%.o Utilities/sequencer/%.su Utilities/sequencer/%.cyclo: ../Utilities/sequencer/%.c Utilities/sequencer/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32WB35xx -DUSE_HAL_DRIVER -c -I../Drivers/STM32WBxx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32WBxx/Include -I../Drivers/CMSIS/Include -I"D:/Eurorack/Ball/Ball_Base_Test/STM_Files" -I"D:/Eurorack/Ball/Ball_Base_Test/src" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/utilities" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/template" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Inc" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/auto" -I"D:/Eurorack/Ball/Ball_Base_Test/STM32_WPAN/App" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/lpm/tiny_lpm" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/sequencer" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Src" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Utilities-2f-sequencer

clean-Utilities-2f-sequencer:
	-$(RM) ./Utilities/sequencer/stm32_seq.cyclo ./Utilities/sequencer/stm32_seq.d ./Utilities/sequencer/stm32_seq.o ./Utilities/sequencer/stm32_seq.su

.PHONY: clean-Utilities-2f-sequencer

