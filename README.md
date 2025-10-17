# Spp Compiler

[![Tests](https://img.shields.io/badge/Tests-1031-green?logo=pytest&logoColor=ffffff)]()
[![Coverage Status](https://img.shields.io/badge/Test%20Coverage-93.50%25%20(964/1031)-cactus?logo=pytest&logoColor=ffffff)]()
[![License](https://img.shields.io/badge/Liscence-S++-orange)](https://github.com/SamG101-Developer/spp/blob/master/LICENSE.md)
[![License Commercial](https://img.shields.io/badge/Liscence-S++%20Commercial-red)](https://github.com/SamG101-Developer/spp/blob/master/LICENSE.md)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)
[![Bugs](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=bugs)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)
[![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=code_smells)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=reliability_rating)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=security_rating)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)
[![Vulnerabilities](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=vulnerabilities)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)
[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=SamG101-Developer_spp&metric=ncloc)](https://sonarcloud.io/summary/new_code?id=SamG101-Developer_spp)

## License Notice (Pre-Release)

The S++ Compiler is currently in development and has not reached its first official release.

- `ast_cast` doesn't work properly for shared pointers; if one dies then they all die, because I copy the raw pointer
  in, instead of casting it using `dynamic_pointer_cast`.


