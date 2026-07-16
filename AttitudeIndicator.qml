import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15


Rectangle {


    id:root



    //========================
    // HUD透明背景
    //========================

    color:"transparent"


    radius:8


    border.width:0







    //========================
    // 姿态数据
    //========================


    property real pitch: backend.pitch

    property real roll: backend.roll

    property real yaw: backend.yaw










    ColumnLayout{


        anchors.fill:parent


        anchors.margins:12


        spacing:8







        Text{


            text:"🧭 姿态仪表"


            font.pixelSize:18


            font.bold:true


            color:"#00ffff"



        }









        Rectangle{


            Layout.fillWidth:true


            Layout.fillHeight:true



            color:"#14181fcc"



            radius:6







            Canvas{


                id:canvas



                anchors.fill:parent



                anchors.margins:10






                onPaint:{



                    var ctx=getContext("2d")



                    var w=width

                    var h=height





                    ctx.clearRect(
                        0,
                        0,
                        w,
                        h
                    )





                    var cx=w/2

                    var cy=h/2







                    ctx.save()







                    //========================
                    // 横滚
                    //========================


                    ctx.translate(
                        cx,
                        cy
                    )





                    ctx.rotate(
                        root.roll*Math.PI/180
                    )









                    //========================
                    // 天空
                    //========================


                    ctx.fillStyle="#154360"


                    ctx.fillRect(

                        -w,

                        -h,

                        w*2,

                        h

                    )









                    //========================
                    // 地面
                    //========================


                    ctx.fillStyle="#6e2c00"


                    ctx.fillRect(

                        -w,

                        0,

                        w*2,

                        h

                    )









                    //========================
                    // 地平线
                    //========================


                    ctx.strokeStyle="#00ffff"


                    ctx.lineWidth=2




                    ctx.beginPath()



                    ctx.moveTo(

                        -w,

                        root.pitch*3

                    )



                    ctx.lineTo(

                        w,

                        root.pitch*3

                    )



                    ctx.stroke()





                    ctx.restore()











                    //========================
                    // 飞机中心符号
                    //========================


                    ctx.fillStyle="#ffd54f"



                    ctx.beginPath()



                    ctx.moveTo(

                        cx,

                        cy-15

                    )



                    ctx.lineTo(

                        cx-18,

                        cy+12

                    )



                    ctx.lineTo(

                        cx+18,

                        cy+12

                    )



                    ctx.closePath()



                    ctx.fill()











                    //========================
                    // 外圈
                    //========================


                    ctx.strokeStyle="#ffffff"


                    ctx.lineWidth=2



                    ctx.beginPath()



                    ctx.arc(

                        cx,

                        cy,

                        Math.min(w,h)/3,

                        0,

                        Math.PI*2

                    )



                    ctx.stroke()



                }



            }



        }









        //========================
        // 数据显示
        //========================


        RowLayout{


            Layout.fillWidth:true



            spacing:12






            Text{


                text:"俯仰"


                color:"#8a9ba8"


            }




            Text{


                text:

                root.pitch.toFixed(1)+"°"


                color:"white"



            }







            Text{


                text:"横滚"


                color:"#8a9ba8"


            }





            Text{


                text:

                root.roll.toFixed(1)+"°"


                color:"white"



            }







            Text{


                text:"偏航"


                color:"#8a9ba8"


            }





            Text{


                text:

                root.yaw.toFixed(1)+"°"


                color:"white"



            }



        }





    }









    //========================
    // 数据更新
    //========================


    Connections{


        target:backend





        function onGpsChanged(){



            root.pitch=
            backend.pitch



            root.roll=
            backend.roll



            root.yaw=
            backend.yaw






            canvas.requestPaint()



        }



    }



}