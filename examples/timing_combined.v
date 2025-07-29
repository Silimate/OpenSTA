// module timing_combined (in, clk, out);
//   input in, clk;
//   output out;
//   wire w1, w2, w3, w4, w5, w6;

//   BUF_X1 u1 (.A(clk), .Z(w1));

//   BUF_X1 u2 (.A(in), .Z(w));
//   DFF_X1 r1 (.D(w1), .CK(w1), .Q(w2));
//   BUF_X1 u3 (.A(w2), .Z(w3));
  
//   BUF_X1 u4 (.A(w3), .Z(w4));

//   BUF_X1 u5 (.A(clk), .Z(w1));
//   DFF_X1 r2 (.D(w4), .CK(w1), .Q(w5));
//   BUF_X1 u6 (.A(w5), .Z(w6));

//   BUF_X1 u7 (.A(w6), .Z(out));

// endmodule