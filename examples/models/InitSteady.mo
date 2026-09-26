model InitSteady
  Real x(start = 1.0);
equation
  der(x) = -x;
initial equation
  der(x) = 0;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));
end InitSteady;
