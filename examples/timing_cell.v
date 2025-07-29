module timing_cell (in, clk, out);
  input in, clk;
  output out;
  wire w1, w2, w3;

  BUF_X1 u1 (.A(clk), .Z(w2));
  
  BUF_X1 u2 (.A(in), .Z(w1));
  DFF_X1 r1 (.D(w1), .CK(w2), .Q(w3));
  BUF_X1 u3 (.A(w3), .Z(out));

endmodule