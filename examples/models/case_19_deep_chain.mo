model Case19DeepChain
  Real s19(start = 1, fixed = true);
  Real u1; Real u2; Real u3; Real u4; Real u5; Real u6;
equation
  der(s19) = -0.01 * u6 * s19;
  u1 = s19 * 1;
  u2 = u1 + 1;
  u3 = u2 + 1;
  u4 = u3 + 1;
  u5 = u4 + 1;
  u6 = u5 + 1;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.01));
end Case19DeepChain;
