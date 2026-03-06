module power_calc_no_inv (
  input in,
  output out
);

  wire mid, mid2;

  BUFx2_ASAP7_75t_R I1 (
    .A(in),
    .Y(mid)
  );

  INVx2_ASAP7_75t_R I2 (
    .A(mid),
    .Y(mid2)
  );

  INVx2_ASAP7_75t_R I3 (
    .A(mid2),
    .Y(out)
  );

endmodule