module blk (ck, d, q);
  input ck, d;
  output q;
  wire d1, d2, q0;

  BUF_X1 b1 (.A(d), .Z(d1));
  BUF_X1 b2 (.A(d1), .Z(d2));
  DFF_X1 r1 (.D(d2), .CK(ck), .Q(q0));
  BUF_X1 b3 (.A(q0), .Z(q));
endmodule

// u_blk1 sits behind long input and output chains, u_blk2 behind short
// ones with an extra load on its output, u_blk3 behind a top input.
module top (clk, in, in2, out1, out2, out3, out4);
  input clk, in, in2;
  output out1, out2, out3, out4;
  wire r_q, a1, a2, a3, a4, q1, q2, c1, c2, c3, c4;

  DFF_X1 r_in (.D(in), .CK(clk), .Q(r_q));
  BUF_X1 a_1 (.A(r_q), .Z(a1));
  BUF_X1 a_2 (.A(a1), .Z(a2));
  BUF_X1 a_3 (.A(a2), .Z(a3));
  BUF_X1 a_4 (.A(a3), .Z(a4));
  blk u_blk1 (.ck(clk), .d(a4), .q(q1));
  blk u_blk2 (.ck(clk), .d(r_q), .q(q2));
  blk u_blk3 (.ck(clk), .d(in2), .q(out4));
  BUF_X1 c_1 (.A(q1), .Z(c1));
  BUF_X1 c_2 (.A(c1), .Z(c2));
  BUF_X1 c_3 (.A(c2), .Z(c3));
  BUF_X1 c_4 (.A(c3), .Z(c4));
  DFF_X1 r_out1 (.D(c4), .CK(clk), .Q(out1));
  DFF_X1 r_out2 (.D(q2), .CK(clk), .Q(out2));
  INV_X1 i_out3 (.A(q2), .ZN(out3));
endmodule
