#ifndef __ARM_FEATURES_H__
#define __ARM_FEATURES_H__

#if defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) \
 || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) \
 || defined(__ARM_ARCH_7EM__)

#define HAVE_ARMV7
#define HAVE_ARMV6
#define HAVE_ARMV5

#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
   || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) \
   || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__)

#define HAVE_ARMV6
#define HAVE_ARMV5

#elif defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5E__) \
   || defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__)

#define HAVE_ARMV5

#endif

/* no need for HAVE_NEON - GCC defines __ARM_NEON__ consistently */

#endif /* __ARM_FEATURES_H__ */
