model Hello
  Real x(start = 0, fixed = true);
equation
  der(x) = 1;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));
end Hello;
