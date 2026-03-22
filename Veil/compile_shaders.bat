@echo off

glslc %~dp0shaders\fillrate.vert -o %~dp0shaders\fillrate_vert.spv
glslc %~dp0shaders\fillrate.frag -o %~dp0shaders\fillrate_frag.spv

glslc %~dp0shaders\bandwidth.comp -o %~dp0shaders\bandwidth_comp.spv

glslc %~dp0shaders\compute.comp -o %~dp0shaders\compute_comp.

glslc %~dp0shaders\stall.comp -o %~dp0shaders\stall_comp.spv