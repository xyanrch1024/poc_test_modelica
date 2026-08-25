/* Main Simulation File */

#if defined(__cplusplus)
extern "C" {
#endif

#include "Hello_model.h"
#include "simulation/solver/events.h"
#include "simulation/arrayIndex.h"

/* FIXME these defines are ugly and hard to read, why not use direct function pointers instead? */
#define prefixedName_performSimulation Hello_performSimulation
#define prefixedName_updateContinuousSystem Hello_updateContinuousSystem
#include <simulation/solver/perform_simulation.c.inc>

#define prefixedName_performQSSSimulation Hello_performQSSSimulation
#include <simulation/solver/perform_qss_simulation.c.inc>


/* dummy VARINFO and FILEINFO */
const VAR_INFO dummyVAR_INFO = omc_dummyVarInfo;

int Hello_input_function(DATA *data, threadData_t *threadData)
{
  
  return 0;
}

int Hello_input_function_init(DATA *data, threadData_t *threadData)
{
  
  return 0;
}

int Hello_input_function_updateStartValues(DATA *data, threadData_t *threadData)
{
  
  return 0;
}

int Hello_inputNames(DATA *data, char ** names){
  
  return 0;
}

int Hello_data_function(DATA *data, threadData_t *threadData)
{
  return 0;
}

int Hello_dataReconciliationInputNames(DATA *data, char ** names){
  
  return 0;
}

int Hello_dataReconciliationUnmeasuredVariables(DATA *data, char ** names)
{
  
  return 0;
}

int Hello_output_function(DATA *data, threadData_t *threadData)
{
  
  return 0;
}

int Hello_setc_function(DATA *data, threadData_t *threadData)
{
  
  return 0;
}

int Hello_setb_function(DATA *data, threadData_t *threadData)
{
  
  return 0;
}


/*
equation index: 3
type: SIMPLE_ASSIGN
$DER.x = 1.0
*/
void Hello_eqFunction_3(DATA *data, threadData_t *threadData)
{
  const int equationIndexes[2] = {1,3};
  (data->localData[0]->realVars[data->simulationInfo->realVarsIndex[1]] /* der(x) STATE_DER */) = 1.0;
  threadData->lastEquationSolved = 3;
}

OMC_DISABLE_OPT
int Hello_functionDAE(DATA *data, threadData_t *threadData)
{
  int equationIndexes[1] = {0};
#if !defined(OMC_MINIMAL_RUNTIME)
  if (measure_time_flag) rt_tick(SIM_TIMER_DAE);
#endif

  data->simulationInfo->needToIterate = 0;
  data->simulationInfo->discreteCall = 1;
  Hello_functionLocalKnownVars(data, threadData);
  static void (*const eqFunctions[1])(DATA*, threadData_t*) = {
    Hello_eqFunction_3
  };
  
  for (int id = 0; id < 1; id++) {
    eqFunctions[id](data, threadData);
  }
  data->simulationInfo->discreteCall = 0;
  
#if !defined(OMC_MINIMAL_RUNTIME)
  if (measure_time_flag) rt_accumulate(SIM_TIMER_DAE);
#endif
  return 0;
}


int Hello_functionLocalKnownVars(DATA *data, threadData_t *threadData)
{
  
  return 0;
}

/* forwarded equations */
extern void Hello_eqFunction_3(DATA* data, threadData_t *threadData);

static void functionODE_system0(DATA *data, threadData_t *threadData)
{
  static void (*const eqFunctions[1])(DATA*, threadData_t*) = {
    Hello_eqFunction_3
  };
  
  if (data->simulationInfo->evalSelection) {
    for (int i = 0; i < data->simulationInfo->evalSelection->n; i++) {
      int id = data->simulationInfo->evalSelection->idx[i];
      eqFunctions[id](data, threadData);
    }
  } else {
    for (int id = 0; id < 1; id++) {
      eqFunctions[id](data, threadData);
    }
  }
}

int Hello_functionODE(DATA *data, threadData_t *threadData)
{
#if !defined(OMC_MINIMAL_RUNTIME)
  if (measure_time_flag) rt_tick(SIM_TIMER_FUNCTION_ODE);
#endif

  
  data->simulationInfo->callStatistics.functionODE++;
  
  Hello_functionLocalKnownVars(data, threadData);
  functionODE_system0(data, threadData);

#if !defined(OMC_MINIMAL_RUNTIME)
  if (measure_time_flag) rt_accumulate(SIM_TIMER_FUNCTION_ODE);
#endif

  return 0;
}

void Hello_ODE_DAG(DATA* data, threadData_t* threadData)
{
  const size_t eqMap[] = {3};
  buildEvalDAG_ODE(data->modelData, sizeof(eqMap)/sizeof(size_t), eqMap);
}

/* forward the main in the simulation runtime */
extern int _main_SimulationRuntime(int argc, char **argv, DATA *data, threadData_t *threadData);
extern int _main_OptimizationRuntime(int argc, char **argv, DATA *data, threadData_t *threadData);

#include "Hello_12jac.h"
#include "Hello_13opt.h"

struct OpenModelicaGeneratedFunctionCallbacks Hello_callback = {
  (int (*)(DATA *, threadData_t *, void *)) Hello_performSimulation,    /* performSimulation */
  (int (*)(DATA *, threadData_t *, void *)) Hello_performQSSSimulation,    /* performQSSSimulation */
  Hello_updateContinuousSystem,    /* updateContinuousSystem */
  Hello_callExternalObjectDestructors,    /* callExternalObjectDestructors */
  NULL,    /* initialNonLinearSystem */
  NULL,    /* initialLinearSystem */
  NULL,    /* initialMixedSystem */
  #if !defined(OMC_NO_STATESELECTION)
  Hello_initializeStateSets,
  #else
  NULL,
  #endif    /* initializeStateSets */
  Hello_initializeDAEmodeData,
  Hello_ODE_DAG,
  Hello_functionODE,
  Hello_functionAlgebraics,
  Hello_functionDAE,
  Hello_functionLocalKnownVars,
  Hello_input_function,
  Hello_input_function_init,
  Hello_input_function_updateStartValues,
  Hello_data_function,
  Hello_output_function,
  Hello_setc_function,
  Hello_setb_function,
  Hello_function_storeDelayed,
  Hello_function_storeSpatialDistribution,
  Hello_function_initSpatialDistribution,
  Hello_updateBoundVariableAttributes,
  Hello_functionInitialEquations,
  GLOBAL_EQUIDISTANT_HOMOTOPY,
  NULL,
  Hello_functionRemovedInitialEquations,
  Hello_updateBoundParameters,
  Hello_checkForAsserts,
  Hello_function_ZeroCrossingsEquations,
  Hello_function_ZeroCrossings,
  Hello_function_updateRelations,
  Hello_zeroCrossingDescription,
  Hello_relationDescription,
  Hello_function_initSample,
  Hello_INDEX_JAC_A,
  Hello_INDEX_JAC_ADJ,
  Hello_INDEX_JAC_B,
  Hello_INDEX_JAC_C,
  Hello_INDEX_JAC_D,
  Hello_INDEX_JAC_F,
  Hello_INDEX_JAC_H,
  Hello_initialAnalyticJacobianA,
  Hello_initialAnalyticJacobianADJ,
  Hello_initialAnalyticJacobianB,
  Hello_initialAnalyticJacobianC,
  Hello_initialAnalyticJacobianD,
  Hello_initialAnalyticJacobianF,
  Hello_initialAnalyticJacobianH,
  Hello_functionJacA_column,
  Hello_functionJacADJ_column,
  Hello_functionJacB_column,
  Hello_functionJacC_column,
  Hello_functionJacD_column,
  Hello_functionJacF_column,
  Hello_functionJacH_column,
  Hello_JacA_DAG,
  Hello_linear_model_frame,
  Hello_linear_model_datarecovery_frame,
  Hello_mayer,
  Hello_lagrange,
  Hello_getInputVarIndicesInOptimization,
  Hello_pickUpBoundsForInputsInOptimization,
  Hello_setInputData,
  Hello_getTimeGrid,
  Hello_symbolicInlineSystem,
  Hello_function_initSynchronous,
  Hello_function_updateSynchronous,
  Hello_function_equationsSynchronous,
  Hello_inputNames,
  Hello_dataReconciliationInputNames,
  Hello_dataReconciliationUnmeasuredVariables,
  NULL,
  NULL,
  NULL,
  NULL,
  -1,
  NULL,
  NULL,
  -1

};

#define _OMC_LIT_RESOURCE_0_name_data "Hello"
#define _OMC_LIT_RESOURCE_0_dir_data "/home/wsl-ubuntu-1/projects/pocmodelica/example/hello_omc"
static const MMC_DEFSTRINGLIT(_OMC_LIT_RESOURCE_0_name,5,_OMC_LIT_RESOURCE_0_name_data);
static const MMC_DEFSTRINGLIT(_OMC_LIT_RESOURCE_0_dir,57,_OMC_LIT_RESOURCE_0_dir_data);

static const MMC_DEFSTRUCTLIT(_OMC_LIT_RESOURCES,2,MMC_ARRAY_TAG) {MMC_REFSTRINGLIT(_OMC_LIT_RESOURCE_0_name), MMC_REFSTRINGLIT(_OMC_LIT_RESOURCE_0_dir)}};
void Hello_setupDataStruc(DATA *data, threadData_t *threadData)
{
  assertStreamPrint(threadData,0!=data, "Error while initialize Data");
  threadData->localRoots[LOCAL_ROOT_SIMULATION_DATA] = data;
  data->callback = &Hello_callback;
  OpenModelica_updateUriMapping(threadData, MMC_REFSTRUCTLIT(_OMC_LIT_RESOURCES));
  data->modelData->modelName = "Hello";
  data->modelData->modelFilePrefix = "Hello";
  data->modelData->modelFileName = "hello.mo";
  data->modelData->resultFileName = NULL;
  data->modelData->modelDir = "/home/wsl-ubuntu-1/projects/pocmodelica/example/hello_omc";
  data->modelData->modelGUID = "{e3aa6656-d7ce-42bd-8f3b-460d8d331695}";
  #if defined(OPENMODELICA_XML_FROM_FILE_AT_RUNTIME)
  data->modelData->initXMLData = NULL;
  data->modelData->modelDataXml.infoXMLData = NULL;
  #else
  #if defined(_MSC_VER) /* handle joke compilers */
  {
  /* for MSVC we encode a string like char x[] = {'a', 'b', 'c', '\0'} */
  /* because the string constant limit is 65535 bytes */
  static const char contents_init[] =
    #include "Hello_init.c"
    ;
  static const char contents_info[] =
    #include "Hello_info.c"
    ;
    data->modelData->initXMLData = contents_init;
    data->modelData->modelDataXml.infoXMLData = contents_info;
  }
  #else /* handle real compilers */
  data->modelData->initXMLData =
  #include "Hello_init.c"
    ;
  data->modelData->modelDataXml.infoXMLData =
  #include "Hello_info.c"
    ;
  #endif /* defined(_MSC_VER) */
  #endif /* defined(OPENMODELICA_XML_FROM_FILE_AT_RUNTIME) */
  data->modelData->modelDataXml.fileName = "Hello_info.json";
  data->modelData->resourcesDir = NULL;
  data->modelData->runTestsuite = 0;
  data->modelData->nStatesArray = 1;
  data->modelData->nDiscreteReal = 0;
  data->modelData->nVariablesRealArray = 2;
  data->modelData->nVariablesIntegerArray = 0;
  data->modelData->nVariablesBooleanArray = 0;
  data->modelData->nVariablesStringArray = 0;
  data->modelData->nParametersRealArray = 0;
  data->modelData->nParametersIntegerArray = 0;
  data->modelData->nParametersBooleanArray = 0;
  data->modelData->nParametersStringArray = 0;
  data->modelData->nParametersReal = 0;
  data->modelData->nParametersInteger = 0;
  data->modelData->nParametersBoolean = 0;
  data->modelData->nParametersString = 0;
  data->modelData->nAliasRealArray = 0;
  data->modelData->nAliasIntegerArray = 0;
  data->modelData->nAliasBooleanArray = 0;
  data->modelData->nAliasStringArray = 0;
  data->modelData->nInputVars = 0;
  data->modelData->nOutputVars = 0;
  data->modelData->nZeroCrossings = 0;
  data->modelData->nSamples = 0;
  data->modelData->nRelations = 0;
  data->modelData->nMathEvents = 0;
  data->modelData->nExtObjs = 0;
  data->modelData->modelDataXml.modelInfoXmlLength = 0;
  data->modelData->modelDataXml.nFunctions = 0;
  data->modelData->modelDataXml.nProfileBlocks = 0;
  data->modelData->modelDataXml.nEquations = 4;
  data->modelData->nMixedSystems = 0;
  data->modelData->nLinearSystems = 0;
  data->modelData->nNonLinearSystems = 0;
  data->modelData->nStateSets = 0;
  data->modelData->nJacobians = 7;
  data->modelData->nOptimizeConstraints = 0;
  data->modelData->nOptimizeFinalConstraints = 0;
  data->modelData->nDelayExpressions = 0;
  data->modelData->nBaseClocks = 0;
  data->modelData->nSpatialDistributions = 0;
  data->modelData->nSensitivityVars = 0;
  data->modelData->nSensitivityParamVars = 0;
  data->modelData->nSetcVars = 0;
  data->modelData->ndataReconVars = 0;
  data->modelData->nSetbVars = 0;
  data->modelData->nRelatedBoundaryConditions = 0;
  data->modelData->linearizationDumpLanguage = OMC_LINEARIZE_DUMP_LANGUAGE_MODELICA;
}

static int rml_execution_failed()
{
  fflush(NULL);
  fprintf(stderr, "Execution failed!\n");
  fflush(NULL);
  return 1;
}


#if defined(__MINGW32__) || defined(_MSC_VER)

#if !defined(_UNICODE)
#define _UNICODE
#endif
#if !defined(UNICODE)
#define UNICODE
#endif

#include <windows.h>
char** omc_fixWindowsArgv(int argc, wchar_t **wargv)
{
  char** newargv;
  /* Support for non-ASCII characters
  * Read the unicode command line arguments and translate it to char*
  */
  newargv = (char**)malloc(argc*sizeof(char*));
  for (int i = 0; i < argc; i++) {
    newargv[i] = omc_wchar_to_multibyte_str(wargv[i]);
  }
  return newargv;
}

#define OMC_MAIN wmain
#define OMC_CHAR wchar_t
#define OMC_EXPORT __declspec(dllexport) extern

#else
#define omc_fixWindowsArgv(N, A) (A)
#define OMC_MAIN main
#define OMC_CHAR char
#define OMC_EXPORT extern
#endif

#if defined(threadData)
#undef threadData
#endif
/* call the simulation runtime main from our main! */
#if defined(OMC_DLL_MAIN_DEFINE)
OMC_EXPORT int omcDllMain(int argc, OMC_CHAR **argv)
#else
int OMC_MAIN(int argc, OMC_CHAR** argv)
#endif
{
  char** newargv = omc_fixWindowsArgv(argc, argv);
  /*
    Set the error functions to be used for simulation.
    The default value for them is 'functions' version. Change it here to 'simulation' versions
  */
  omc_assert = omc_assert_simulation;
  omc_assert_withEquationIndexes = omc_assert_simulation_withEquationIndexes;

  omc_assert_warning_withEquationIndexes = omc_assert_warning_simulation_withEquationIndexes;
  omc_assert_warning = omc_assert_warning_simulation;
  omc_terminate = omc_terminate_simulation;
  omc_throw = omc_throw_simulation;

  int res;
  DATA data;
  MODEL_DATA modelData;
  SIMULATION_INFO simInfo;
  data.modelData = &modelData;
  data.simulationInfo = &simInfo;
  measure_time_flag = 0;
  compiledInDAEMode = 0;
  compiledWithSymSolver = 0;
  MMC_INIT(0);
  omc_alloc_interface.init();
  {
    MMC_TRY_TOP()
  
    MMC_TRY_STACK()
  
    Hello_setupDataStruc(&data, threadData);
    res = _main_initRuntimeAndSimulation(argc, newargv, &data, threadData);
    if(res == 0) {
      if (omc_flag[FLAG_MOO_OPTIMIZATION]) {
        res = _main_OptimizationRuntime(argc, newargv, &data, threadData);
      } else {
        res = _main_SimulationRuntime(argc, newargv, &data, threadData);
      }
    }
    
    MMC_ELSE()
    rml_execution_failed();
    fprintf(stderr, "Stack overflow detected and was not caught.\nSend us a bug report at https://trac.openmodelica.org/OpenModelica/newticket\n    Include the following trace:\n");
    printStacktraceMessages();
    fflush(NULL);
    return 1;
    MMC_CATCH_STACK()
    
    MMC_CATCH_TOP(return rml_execution_failed());
  }

  fflush(NULL);
  return res;
}

#ifdef __cplusplus
}
#endif


