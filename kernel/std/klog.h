#ifndef KLOG_H
#define KLOG_H

#define KLOG(func, ...) ({printk_dbg("KLog: %s:%d calls %s", __FILE__, __LINE__, #func); func(__VA_ARGS__);})

#endif
