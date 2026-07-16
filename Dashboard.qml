import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15


Rectangle {


    id:root



    //========================
    // 透明背景
    //========================

    color:"transparent"


    radius:8


    border.width:0





    property string flightStatus:"待命"







    // 根据内容自动调整高度

    height:layout.height+24







    ColumnLayout{


        id:layout



        anchors.left:parent.left

        anchors.right:parent.right

        anchors.top:parent.top



        anchors.margins:12



        spacing:8







        //========================
        // 标题
        //========================


        Text{


            text:"✈ 飞行状态"



            color:"#00ffff"



            font.pixelSize:18



            font.bold:true



        }









        //========================
        // 当前状态
        //========================


        Rectangle{


            Layout.fillWidth:true



            height:30



            radius:5






            color:

            root.flightStatus==="飞行中"

            ?

            "#2e7d32"


            :


            root.flightStatus==="待命"


            ?


            "#f57f17"


            :


            "#b71c1c"






            Text{


                anchors.centerIn:parent



                text:root.flightStatus



                color:"white"



                font.pixelSize:14



                font.bold:true



            }



        }









        //========================
        // 飞行参数
        //========================


        GridLayout{


            Layout.fillWidth:true



            columns:2



            columnSpacing:15


            rowSpacing:6







            Text{


                text:"高度:"


                color:"#8a9ba8"


            }



            Text{


                text:


                Number(backend.height || 0)
                .toFixed(1)

                +" m"



                color:"white"



            }









            Text{


                text:"速度:"


                color:"#8a9ba8"


            }



            Text{


                text:


                Number(backend.speed || 0)
                .toFixed(1)

                +" m/s"



                color:"white"



            }









            Text{


                text:"电量:"


                color:"#8a9ba8"


            }



            Text{


                text:


                Number(backend.battery || 0)
                .toFixed(1)

                +" %"



                color:"white"



            }









            Text{


                text:"纬度:"


                color:"#8a9ba8"



            }




            Text{


                text:


                Number(
                backend.latitude || 0
                )
                .toFixed(8)



                color:"white"



            }









            Text{


                text:"经度:"


                color:"#8a9ba8"



            }




            Text{


                text:


                Number(
                backend.longitude || 0
                )
                .toFixed(8)



                color:"white"



            }









            Text{


                text:"时间:"


                color:"#8a9ba8"



            }





            Text{


                text:


                backend.gpsTime || "--"



                color:"white"



            }





        }









        //========================
        // 控制按钮
        //========================


        GridLayout{


            Layout.fillWidth:true



            columns:3



            columnSpacing:8







            Button{


                Layout.fillWidth:true



                text:"起飞"




                background:


                Rectangle{


                    color:"#2e7d32"



                    radius:5



                }






                onClicked:{


                    root.flightStatus="飞行中"


                }



            }









            Button{


                Layout.fillWidth:true



                text:"悬停"





                background:


                Rectangle{


                    color:"#f57f17"



                    radius:5



                }





                onClicked:{


                    root.flightStatus="悬停"


                }



            }









            Button{


                Layout.fillWidth:true



                text:"降落"






                background:


                Rectangle{


                    color:"#b71c1c"



                    radius:5



                }






                onClicked:{


                    root.flightStatus="降落"


                }



            }




        }




    }




}