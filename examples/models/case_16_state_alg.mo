model Case16StateAlg
  Real p(start = 1, fixed = true);
  Real q16;
equation
  der(p) = q16 + p;
  q16 = p * 0.25;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.01));
end Case16StateAlg;
