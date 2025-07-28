module timing_cell_clock (in, clk, out);
  input in, clk;
  output out;
  wire w1, w2, w3, w4, w5, w6;

  BUF_X1 u1 (.A(clk), .Z(w1));

  DFF_X1 r1 (.D(in), .CK(w1), .Q(w2));

  BUF_X1 u2 (.A(w2), .Z(w3));
  BUF_X1 u3 (.A(w3), .Z(w4));

  DFF_X1 r2 (.D(w4), .CK(w1), .Q(w5));

  BUF_X1 u2 (.A(w5), .Z(w6));
  BUF_X1 u3 (.A(w6), .Z(out));

endmodule