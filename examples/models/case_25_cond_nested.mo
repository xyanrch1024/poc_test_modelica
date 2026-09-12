model Case25CondNested
  Real s(start = 2, fixed = true);
  Real w;
equation
  w = if s < 2 then (if s < 1 then 1 else 2) else 3;
  if s < 0.5 then
    der(s) = -2;
  else
    der(s) = -1;
  end if;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.1));
end Case25CondNested;