#if defined(__cplusplus)
  extern "C" {
#endif
  int Hello_mayer(DATA* data, modelica_real** res, short*);
  int Hello_lagrange(DATA* data, modelica_real** res, short *, short *);
  int Hello_getInputVarIndicesInOptimization(DATA* data, int* input_var_indices);
  int Hello_pickUpBoundsForInputsInOptimization(DATA* data, modelica_real* min, modelica_real* max, modelica_real*nominal, modelica_boolean *useNominal, char ** name, modelica_real * start, modelica_real * startTimeOpt);
  int Hello_setInputData(DATA *data, const modelica_boolean file);
  int Hello_getTimeGrid(DATA *data, modelica_integer * nsi, modelica_real**t);
#if defined(__cplusplus)
}
#endif
