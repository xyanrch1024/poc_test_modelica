model Case02Cooling
  parameter Real kc = 0.3;
  parameter Real Te = 5;
  Real T(start = 40, fixed = true);
equation
  der(T) = -kc * (T - Te);
  annotation(experiment(StartTime = 0, StopTime = 3, Interval = 0.03));
end Case02Cooling;
