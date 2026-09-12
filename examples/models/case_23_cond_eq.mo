model Case23CondEq
  Real q;
  Real r;
  Real s(start = 0, fixed = true);
equation
  if s < 1 then
    q = 2 * s;
  else
    q = s + 1;
  end if;
  r = if s < 1 then q else -q;
  der(s) = if s < 1 then 1 else 2;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.01));
end Case23CondEq;