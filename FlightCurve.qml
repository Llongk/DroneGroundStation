import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15


Rectangle {


    id:root


    color:"transparent"


    radius:8


    border.width:0



    property var dataPoints:[]

    property string dataType:"alt"





    ColumnLayout{


        anchors.fill:parent

        anchors.margins:12

        spacing:8







        //========================
        // 标题栏
        //========================


        RowLayout{


            Layout.fillWidth:true




            Text{


                text:"📊 飞行数据曲线"


                color:"#00ffff"


                font.pixelSize:18


                font.bold:true


            }






            Item{


                Layout.fillWidth:true


            }







            ComboBox{


                id:typeCombo



                model:[

                    "高度",

                    "速度",

                    "温度",

                    "湿度"

                ]





                onCurrentIndexChanged:{


                    root.dataType =

                    [

                        "alt",

                        "speed",

                        "temp",

                        "humi"

                    ][currentIndex]



                    canvas.requestPaint()

                }


            }


        }









        //========================
        // 曲线区域
        //========================


        Rectangle{


            Layout.fillWidth:true


            Layout.fillHeight:true



            color:"#14181fcc"



            radius:6






            Canvas{


                id:canvas



                anchors.fill:parent



                anchors.margins:10







                function getValue(p){


                    if(root.dataType==="alt")

                        return p.alt



                    if(root.dataType==="speed")

                        return p.speed



                    if(root.dataType==="temp")

                        return p.temp



                    if(root.dataType==="humi")

                        return p.humi



                    return 0


                }







                function getUnit(){


                    if(root.dataType==="alt")

                        return "高度(m)"



                    if(root.dataType==="speed")

                        return "速度(m/s)"



                    if(root.dataType==="temp")

                        return "温度(℃)"



                    if(root.dataType==="humi")

                        return "湿度(%)"



                    return ""

                }








                onPaint:{



                    var ctx=getContext("2d")



                    ctx.clearRect(
                        0,
                        0,
                        width,
                        height
                    )






                    // 没有数据不绘制

                    if(root.dataPoints.length < 2)

                    {

                        return

                    }







                    var left=50

                    var right=20

                    var top=20

                    var bottom=35






                    var chartWidth =
                    width-left-right



                    var chartHeight =
                    height-top-bottom







                    var maxValue=1







                    for(
                        var i=0;
                        i<root.dataPoints.length;
                        i++
                    )

                    {


                        var value=getValue(
                        root.dataPoints[i]
                        )



                        if(value>maxValue)

                            maxValue=value


                    }






                    maxValue*=1.2







                    //====================
                    // 坐标轴
                    //====================


                    ctx.strokeStyle="#607d8b"

                    ctx.lineWidth=1



                    ctx.beginPath()



                    ctx.moveTo(
                        left,
                        top
                    )



                    ctx.lineTo(
                        left,
                        height-bottom
                    )



                    ctx.lineTo(
                        width-right,
                        height-bottom
                    )



                    ctx.stroke()







                    //====================
                    // Y轴
                    //====================


                    ctx.fillStyle="#8a9ba8"


                    ctx.font="12px sans-serif"






                    for(
                        var n=0;
                        n<=5;
                        n++
                    )

                    {



                        var y =

                        height-bottom
                        -
                        n*chartHeight/5






                        var valueText =

                        maxValue*n/5






                        ctx.fillText(

                            valueText.toFixed(1),

                            5,

                            y+4

                        )


                    }








                    ctx.fillText(

                        getUnit(),

                        5,

                        15

                    )









                    //====================
                    // 曲线
                    //====================


                    ctx.strokeStyle="#00ffff"


                    ctx.lineWidth=2



                    ctx.beginPath()





                    for(
                        var j=0;
                        j<root.dataPoints.length;
                        j++
                    )

                    {



                        var x =

                        left
                        +
                        j*
                        chartWidth/
                        (
                        root.dataPoints.length-1
                        )






                        var y =

                        height-bottom
                        -
                        getValue(
                        root.dataPoints[j]
                        )
                        /
                        maxValue
                        *
                        chartHeight






                        if(j===0)

                            ctx.moveTo(
                            x,
                            y
                            )

                        else

                            ctx.lineTo(
                            x,
                            y
                            )



                    }






                    ctx.stroke()



                }



            }


        }









        //========================
        // 当前数据
        //========================


        RowLayout{


            Layout.fillWidth:true





            Item{


                Layout.fillWidth:true


            }






            Text{


                visible:
                root.dataPoints.length>0




                text:

                "当前: "

                +

                getCurrentValue()





                color:"white"



            }




        }



    }









    //========================
    // 获取当前值
    //========================


    function getCurrentValue(){


        if(root.dataPoints.length===0)

            return ""



        var p=
        root.dataPoints[
        root.dataPoints.length-1
        ]




        if(root.dataType==="alt")

            return p.alt.toFixed(1)+" m"



        if(root.dataType==="speed")

            return p.speed.toFixed(1)+" m/s"



        if(root.dataType==="temp")

            return p.temp.toFixed(1)+" ℃"



        if(root.dataType==="humi")

            return p.humi.toFixed(1)+" %"



        return ""

    }









    //========================
    // 后端数据
    //========================


    Connections{


        target:backend





        function onGpsChanged(){


            addPoint()


        }



    }

    Connections{

        target:sensorBackend

        function onSensorChanged(){

            addPoint()

        }

    }










    function addPoint(){



        var p={



            alt:backend.height,



            speed:backend.speed,



            temp:sensorBackend.temperature,



            humi:sensorBackend.humidity



        }






        root.dataPoints.push(p)







        if(root.dataPoints.length>200)


            root.dataPoints.shift()






        canvas.requestPaint()



    }



}
