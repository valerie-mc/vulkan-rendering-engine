# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-src")
  file(MAKE_DIRECTORY "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-src")
endif()
file(MAKE_DIRECTORY
  "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-build"
  "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-subbuild/ktx-populate-prefix"
  "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-subbuild/ktx-populate-prefix/tmp"
  "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-subbuild/ktx-populate-prefix/src/ktx-populate-stamp"
  "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-subbuild/ktx-populate-prefix/src"
  "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-subbuild/ktx-populate-prefix/src/ktx-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-subbuild/ktx-populate-prefix/src/ktx-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/val/Programming/C++/Vulkan-Tutorial/attachments/template/build/_deps/ktx-subbuild/ktx-populate-prefix/src/ktx-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
