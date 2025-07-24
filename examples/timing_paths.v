module timing_paths (in, clk, out);
  input in, clk;
  output out;
  wire w1, w2;

  BUF_X1 u1 (.A(in), .Z(w1));
  DFF_X1 r1 (.D(w1), .CK(clk), .Q(w2));
  BUF_X1 u2 (.A(w2), .Z(out));
endmodule // top
