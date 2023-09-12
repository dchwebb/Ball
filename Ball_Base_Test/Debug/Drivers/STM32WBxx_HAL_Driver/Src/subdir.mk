################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.c 

C_DEPS += \
./Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.d 

OBJS += \
./Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.o 


# Each subdirectory must supply rules for building sources it contributes
Drivers/STM32WBxx_HAL_Driver/Src/%.o Drivers/STM32WBxx_HAL_Driver/Src/%.su Drivers/STM32WBxx_HAL_Driver/Src/%.cyclo: ../Drivers/STM32WBxx_HAL_Driver/Src/%.c Drivers/STM32WBxx_HAL_Driver/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32WB35xx -DUSE_HAL_DRIVER -c -I../Drivers/STM32WBxx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32WBxx/Include -I../Drivers/CMSIS/Include -I"D:/Eurorack/Ball/Ball_Base_Test/STM_Files" -I"D:/Eurorack/Ball/Ball_Base_Test/src" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/utilities" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/template" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Inc" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/core/auto" -I"D:/Eurorack/Ball/Ball_Base_Test/STM32_WPAN/App" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/lpm/tiny_lpm" -I"D:/Eurorack/Ball/Ball_Base_Test/Utilities/sequencer" -I"D:/Eurorack/Ball/Ball_Base_Test/Middlewares/ST/STM32_WPAN/ble/svc/Src" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-STM32WBxx_HAL_Driver-2f-Src

clean-Drivers-2f-STM32WBxx_HAL_Driver-2f-Src:
	-$(RM) ./Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.cyclo ./Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.d ./Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.o ./Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.su

.PHONY: clean-Drivers-2f-STM32WBxx_HAL_Driver-2f-Src

