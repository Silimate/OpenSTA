module timing_paths (in, clk, out);
  input in, clk;
  output out;
  wire w1, w2;

  DFF_X1 r1 (.D(in), .CK(clk), .Q(w2));
  BUF_X1 u2 (.A(w2), .Z(out));
endmodule // top
