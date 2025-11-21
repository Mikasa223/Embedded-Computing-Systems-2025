################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccs1260/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --float_support=softlib --tmu_support=tmu0 --include_path="C:/Users/lm21674/Downloads/OneDrive_1_17-11-2025/part_3_FFT" --include_path="C:/ti/c2000/C2000Ware_5_02_00_00/device_support/f2837xd/common/source" --include_path="C:/ti/c2000/C2000Ware_5_02_00_00/device_support/f2837xd/common/include" --include_path="C:/ti/c2000/C2000Ware_5_02_00_00/device_support/f2837xd/headers/include" --include_path="C:/ti/ccs1260/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --advice:performance=all --define=CPU1 -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


