/* Initialization */
#include "Hello_model.h"
#include "Hello_11mix.h"
#include "Hello_12jac.h"
#if defined(__cplusplus)
extern "C" {
#endif

void Hello_functionInitialEquations_0(DATA *data, threadData_t *threadData);
extern void Hello_eqFunction_3(DATA *data, threadData_t *threadData);


/*
equation index: 2
type: SIMPLE_ASSIGN
x = $START.x
*/
void Hello_eqFunction_2(DATA *data, threadData_t *threadData)
{
  const int equationIndexes[2] = {1,2};
  (data->localData[0]->realVars[data->simulationInfo->realVarsIndex[0]] /* x STATE(1) */) = ((modelica_real *)((data->modelData->realVarsData[0] /* x STATE(1) */).attribute .start.data))[0];
  threadData->lastEquationSolved = 2;
}
OMC_DISABLE_OPT
void Hello_functionInitialEquations_0(DATA *data, threadData_t *threadData)
{
  static void (*const eqFunctions[2])(DATA*, threadData_t*) = {
    Hello_eqFunction_3,
    Hello_eqFunction_2
  };
  
  for (int id = 0; id < 2; id++) {
    eqFunctions[id](data, threadData);
  }
}

int Hello_functionInitialEquations(DATA *data, threadData_t *threadData)
{
  data->simulationInfo->discreteCall = 1;
  Hello_functionInitialEquations_0(data, threadData);
  data->simulationInfo->discreteCall = 0;
  
  return 0;
}

/* No Hello_functionInitialEquations_lambda0 function */

int Hello_functionRemovedInitialEquations(DATA *data, threadData_t *threadData)
{
  const int *equationIndexes = NULL;
  double res = 0.0;

  
  return 0;
}


#if defined(__cplusplus)
}
#endif
