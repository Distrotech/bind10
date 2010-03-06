from  xfrin import *

xfrin = XfrinConnection('sd.cn.', '218.241.108.122', 12345)
data = xfrin.do_xfrin(False) 
print(data)

