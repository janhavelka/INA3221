set(INA3221_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")

idf_component_register(
  SRCS "${INA3221_REPOSITORY_ROOT}/src/INA3221.cpp"
  INCLUDE_DIRS "${INA3221_REPOSITORY_ROOT}/include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
