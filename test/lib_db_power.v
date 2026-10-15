module lib_db_power (
  input ck,
  input d,
  output y
);

  wire q;

  SDFF r1 (
    .D(d),
    .CK(ck),
    .Q(q)
  );

  BUF u1 (
    .A(q),
    .Y(y)
  );

endmodule
