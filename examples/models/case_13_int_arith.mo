model Case13IntArith
  constant Integer aI = 3;
  constant Integer bI = 4;
  Real s13(start = 1, fixed = true);
  Integer n13;
equation
  der(s13) = -s13;
  n13 = aI * bI + 1;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.05));
end Case13IntArith;
