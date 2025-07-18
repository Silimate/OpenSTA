module timing_test (in1, in2, in3, clk1, clk2, clk3, out1, out2);
  input in1, in2, in3, clk1, clk2, clk3;
  output out1, out2;
  wire r1q, r2q, u1z, u2z, u3z;

  BUF_X1 u3 (.A(in3), .Z(u3z));
  BUF_X1 u4 (.A(u3z), .Z(out2));

  DFF_X1 r1 (.D(in1), .CK(clk1), .Q(r1q));
  DFF_X1 r2 (.D(in2), .CK(clk2), .Q(r2q));
  BUF_X1 u1 (.A(r2q), .Z(u1z));
  AND2_X1 u2 (.A1(r1q), .A2(u1z), .ZN(u2z));
  DFF_X1 r3 (.D(u2z), .CK(clk3), .Q(out1));
endmodule // top
