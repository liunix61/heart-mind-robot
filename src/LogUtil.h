#ifndef LOGUTIL_H
#define LOGUTIL_H

#include <qdebug.h>

#ifdef CF_DEBUG
#ifdef _MSC_VER
#define CF_LOG_DEBUG(fmt, ...) qDebug("[%s,%d] DEBUG:" fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define CF_LOG_DEBUG(fmt, ...) qDebug("[%s,%d] DEBUG:" fmt "\n",__PRETTY_FUNCTION__,__LINE__,##__VA_ARGS__)
#endif
#else
#define CF_LOG_DEBUG(fmt, ...)
#endif

#ifdef _MSC_VER
#define CF_LOG_INFO(fmt, ...) qDebug("[%s,%d] INFO:" fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)
#define CF_LOG_ERROR(fmt, ...) qDebug("[%s,%d] ERROR:" fmt "\n",__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define CF_LOG_INFO(fmt, ...) qDebug("[%s,%d] INFO:" fmt "\n",__PRETTY_FUNCTION__,__LINE__,##__VA_ARGS__)
#define CF_LOG_ERROR(fmt, ...) qDebug("[%s,%d] ERROR:" fmt "\n",__PRETTY_FUNCTION__,__LINE__,##__VA_ARGS__)
#endif

#endif // LOGUTIL_H
