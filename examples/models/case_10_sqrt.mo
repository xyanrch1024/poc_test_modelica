model Case10Sqrt
  Real q(start = 0, fixed = true);
equation
  der(q) = sqrt(q + 1) - 0.5 * q;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case10Sqrt;
