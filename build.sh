#!/bin/bash

source ~/VulkanSDK/1.4.350.1/setup-env.sh
odin build . -debug
slangc shader.slangc -target spirv -profile glsl_460 -o shader.spv

./Ronin
