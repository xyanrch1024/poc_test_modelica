model Case11Parens
  Real z1(start = 1, fixed = true);
equation
  der(z1) = -(z1 * (z1 + 2)) / (z1 + 3) + 1;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case11Parens;
