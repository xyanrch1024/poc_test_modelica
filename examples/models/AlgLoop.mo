model AlgLoop
  Real s(start = 1, fixed = true);
  Real a;
  Real b;
equation
  der(s) = a + b;
  a = b + 1;
  b = a * 0.5;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));
end AlgLoop;
