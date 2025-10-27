if(APPLE)
target_sources(HeartMindRobot
        PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppAllocator.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppDefine.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppDelegate.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/LAppWavFileHandler.mm
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppLive2DManager.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppModel.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppPal.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppSprite.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppTextureManager.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppView.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/main.mm
      ${CMAKE_CURRENT_SOURCE_DIR}/TouchManager.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/GlWidget.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/GlWidget.h
      ${CMAKE_CURRENT_SOURCE_DIR}/EventHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.mm
      ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.h
      ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.ui
      ${CMAKE_CURRENT_SOURCE_DIR}/MessageQueue.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/ResourceLoader.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/TouchManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioUtil.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioUtil.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkUtil.h
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkUtil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MouseEvent.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/MouseEvent.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncMessageQueue.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncMessageQueue.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/ConversationWorker.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/ConversationWorker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ThreadSafeUIUpdater.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ThreadSafeUIUpdater.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncStateManager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncStateManager.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/AsyncIntegrationExample.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/AsyncIntegrationExample.cpp
  )
else()
target_sources(HeartMindRobot
        PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppAllocator.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppDefine.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppDelegate.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppLive2DManager.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppModel.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppPal.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppSprite.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppTextureManager.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/LAppView.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/TouchManager.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/GlWidget.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/GlWidget.h
      ${CMAKE_CURRENT_SOURCE_DIR}/EventHandler.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.h
      ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.ui
      ${CMAKE_CURRENT_SOURCE_DIR}/MessageQueue.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/ResourceLoader.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/TouchManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioUtil.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AudioUtil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/OpusDecoder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/OpusDecoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkUtil.h
        ${CMAKE_CURRENT_SOURCE_DIR}/NetworkUtil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/WebSocketChatDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/WebSocketChatDialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MouseEvent.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/MouseEvent.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncMessageQueue.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncMessageQueue.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/ConversationWorker.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/ConversationWorker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ThreadSafeUIUpdater.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ThreadSafeUIUpdater.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncStateManager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/AsyncStateManager.cpp
        # ${CMAKE_CURRENT_SOURCE_DIR}/AsyncIntegrationExample.h
        # ${CMAKE_CURRENT_SOURCE_DIR}/AsyncIntegrationExample.cpp
  )
endif()
