module top (in1, in2, in3, clk1, clk2, clk3, out1, out2);
  
  input in1, in2, in3, clk1, clk2, clk3;
  output out1, out2;
  wire w1, w2, w3;

  BUF_X1 b1 (.A(in1), .Z(w1));
  BUF_X1 b2 (.A(in2), .Z(w2));
  BUF_X1 b3 (.A(in3), .Z(w3));

  timing_test tt0(.in1(w1), .in2(w2), .in3(w3), .clk1(clk1), .clk2(clk2), .clk3(clk3), .out1(out1), .out2(out2));

endmodule // top
