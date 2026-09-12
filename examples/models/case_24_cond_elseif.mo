model Case24CondElseif
  Real v;
  Real s(start = 0, fixed = true);
equation
  if s < 1 then
    v = 1;
  elseif s < 2 then
    v = 2;
  else
    v = 3;
  end if;
  der(s) = 0.5 * v;
  annotation(experiment(StartTime = 0, StopTime = 4, Interval = 0.1));
end Case24CondElseif;