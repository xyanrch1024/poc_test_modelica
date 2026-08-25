model Diverge
  Real x(start = 3, fixed = true);
equation
  der(x) = x * x;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.01));
end Diverge;
