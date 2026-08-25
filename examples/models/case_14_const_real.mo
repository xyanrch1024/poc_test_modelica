model Case14ConstReal
  constant Real c14 = 2.5;
  Real x14(start = 1, fixed = true);
equation
  der(x14) = -c14 * x14;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case14ConstReal;
