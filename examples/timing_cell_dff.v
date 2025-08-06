module timing_cell_dff (in, clk, out);

  input in, clk;
  output out;
  wire w1, w2, w3, w4;

  BUF_X1 u1 (.A(in), .Z(w1));
  
  DFF_X1 r1 (.D(w1), .CK(clk), .Q(w2));

  BUF_X1 u2 (.A(w2), .Z(w3));
  BUF_X1 u3 (.A(w3), .Z(w4));
  
  DFF_X1 r2 (.D(w4), .CK(clk), .Q(out));

endmodule
