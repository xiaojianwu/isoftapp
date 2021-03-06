/*                                                                              
 * Copyright (C) 2014 AnthonOS Open Source Community                               
 *               2014 - 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 */

import QtQuick 2.2
import QtQuick.Controls 1.0
import cn.com.isoft.qjade 2.0
import "global.js" as Global

Item {
    id: hotTodayView
    width: parent.width

    IconModel {
        id: iconModel
        onError: {
            myLoader.isVisible = false;
            myResultText.isVisible = true;
            Global.isNetworkAvailable = false;
        }
        onIconChanged: {
            myLoader.isVisible = false;
            Global.isNetworkAvailable = true;
        }
    }

    GridView {
        id: iconView
        model: iconModel.icons
        width: parent.width - 30 //668
        height: parent.height - slideShow.height
        anchors.top: slideShow.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        cellWidth: 167; cellHeight: 85
        focus: true
        anchors.left: parent.left
        anchors.leftMargin: 50 // 每个软件包的左边开始位置
        
        delegate: Item {
            width: parent.width; height: parent.height
            anchors.leftMargin: 11

            Image {
                id: appIcon 
                source: modelData.icon 
                asynchronous: true
                sourceSize.width: 48; sourceSize.height: 48

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        stackView.push({item: Qt.resolvedUrl("PackageInfoView.qml"), 
                        properties: {packageName:modelData.name,packageTitle: modelData.title,
                                     stackView: stackView}})
                    }
                }
            }

            Text { 
                id: nameText
                text: modelData.title // + "abccc"
                anchors.top: appIcon.top
                anchors.topMargin: 5
                anchors.left: appIcon.right
                anchors.leftMargin: 11
                font.pixelSize: 13
                width: 100
                clip: true
                elide: Text.ElideRight

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        stackView.push({item: Qt.resolvedUrl("PackageInfoView.qml"), 
                        properties: {packageName:modelData.name,packageTitle: modelData.title,
                                     stackView: stackView}});
                    }
                }
            }

            // 安装时，显示此进度条
            ProgressBar {
                id: progressInfo
                width:  appIcon.width
                anchors.left: appIcon.left
                y: appIcon.y + appIcon.height - 15
                maximumValue:  100
                value : 0
                visible: false
            }

            JadedBus {
                id: jadedBus
                
                property bool isError: false
                property string info: "UnknownInfo"

                Component.onCompleted: {
                    jadedBus.info = jadedBus.getInfo(modelData.name)
                    if (jadedBus.info == "UnknownInfo") {
                        funcButton.visible = false;
                    } else if (jadedBus.info == "InfoRunning") {
                        funcButton.visible = false;
                        infoText.visible = true;
                        infoText.text = qsTr("Running");
                    } else if (jadedBus.info == "InfoWaiting") {
                        funcButton.visible = false;
                        infoText.visible = true;
                        infoText.text = qsTr("Waiting")
                    } else if (jadedBus.info == "InfoInstalled") {
                        funcButton.visible = false
                        infoText.visible = true
                    } else if (jadedBus.info == "InfoUpdatable") {
                        funcButton.text = qsTr("Update")
                    } else if (jadedBus.info == "InfoAvailable") {
                        funcButton.visible = true
                        funcButton.text = qsTr("Install")
                        infoText.visible = false
                    }
                }
                onErrored: {
                    if (name == modelData.name) {
                        jadedBus.isError = true;
                        nameText.text = modelData.title + " (" + qsTr("Error") + ")"
                        funcButton.visible = true;
                        infoText.visible = false;
                    }
                }
                onTaskChanged: {                                               
                    if (name == modelData.name) {
                        jadedBus.info = jadedBus.getInfo(name)
                        if (jadedBus.info == "InfoInstalled") {
                            if (jadedBus.isError) {
                                funcButton.visible = true;
                                infoText.visible = false;
                            } else {
                                funcButton.visible = false; 
                                infoText.visible = true;
                                infoText.text = qsTr("Installed");
                            }
                        } else if (jadedBus.info == "InfoAvailable") {
                            funcButton.visible = true
                            funcButton.text = qsTr("Install")
                            infoText.visible = false
                        }

                        progressInfo.visible = false
                    }                                                          
                }

                // 安装时 接收到的进度
                onPerChanged: {
                    if (name == modelData.name) {
                        progressInfo.value = perCent;
                        if (perCent != 100) {
                            funcButton.visible = false
                            infoText.visible = true
                            infoText.text = qsTr("Waiting")
                            progressInfo.visible = true

                        }

                    }
                }
            }

            PercentageButton {
                id: funcButton
                text: qsTr("Install")
                anchors.top: nameText.bottom
                anchors.topMargin: 9
                anchors.left: nameText.left
                onClicked: {
                    if (funcButton.text == qsTr("Install")) {      
                        jadedBus.install(modelData.name) 
                    } else if (funcButton.text == qsTr("Update")) {             
                        jadedBus.update(modelData.name)                        
                    }
                    funcButton.visible = false
                    infoText.visible = true
                    infoText.text = qsTr("Waiting")
                }
            }

            Text {
                id: infoText 
                text: qsTr("Installed")
                color: "#999999"
                anchors.top: funcButton.top
                anchors.left: funcButton.left
                visible: false
            }
        }
    }

    SlideShowModel {                                                               
        id: slideshowModel                                                         
        onError: {
            slideShow.visible = false;
        }
    }                                                                              
                                                                                   
    SlideShow {                                                                    
        id: slideShow                                                              
        slideModel: slideshowModel.slideshow                                       
        width: parent.width - 30 //668
        height: 168
        anchors.top: parent.top                                                    
        anchors.margins: 8                                                         
        anchors.horizontalCenter: parent.horizontalCenter                          
    }

    //------------------------------------------------------------------------- 
    // FIXME: For cover the left and right part for slideshow, you can simply 
    // comment it to experience the issue
    //------------------------------------------------------------------------- 
    Rectangle {                                                                    
        width: (parent.width - slideShow.width) / 2                                
        height: 200                                                                
        color: "white"                                                             
        anchors.top: parent.top                                                    
        anchors.right: parent.right                                                
    }                                                                              
                                                                                   
    Rectangle {                                                                    
        width: (parent.width - slideShow.width) / 2                                
        height: 200                                                                
        color: "white"                                                             
        anchors.top: parent.top                                                    
        anchors.left: parent.left                                                  
    }

    MyLoader { id: myLoader }

    MyResultText { id: myResultText; result: qsTr("Network is Unavailable") }
}
