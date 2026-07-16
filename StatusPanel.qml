import QtQuick
import QtQuick.Controls


Rectangle {


    color:"transparent"


    property var abnormalList:[]
    property int updateFlag:0


    property string flightId:""



    Column {


        anchors.fill:parent

        anchors.margins:20

        height: childrenRect.height


        spacing:10



        Text{


            text:"轨迹异常状态"


            color:"white"


            font.pixelSize:24


            font.bold:true


        }



        Text{


            text:

            "飞行记录:"
            +
            flightId


            color:"#00ffff"


            font.pixelSize:16


        }



        Text{


            text:

            "异常数量:"
            +
            abnormalList.length


            color:"#ff5555"


            font.pixelSize:18


            font.bold:true


        }





        Rectangle{


            width:parent.width


            height:
            parent.height-100


            color:"transparent"

            ScrollView{


                anchors.fill:parent


                Column{


                    width:parent.width


                    spacing:12


                    height:childrenRect.height



                    Repeater{


                        model:updateFlag >= 0 ? abnormalList : []



                        delegate:Rectangle{


                            width:parent.width


                            height:110


                            radius:8


                            color:"#223344"



                            Column{


                                anchors.fill:parent


                                anchors.margins:10


                                spacing:5



                                Text{


                                    text:
                                    "异常点 "
                                    +
                                    (index+1)


                                    color:"#ffd54f"


                                    font.pixelSize:16

                                }



                                Text{


                                    text:
                                    "时间:"
                                    +
                                    modelData["time"]


                                    color:"white"

                                }



                                Text{


                                    text:
                                    "事件:"
                                    +
                                    modelData["event"]


                                    color:"#ff5555"

                                }



                                Text{


                                    text:
                                    "位置:"
                                    +
                                    modelData["latitude"]
                                    +
                                    ","
                                    +
                                    modelData["longitude"]


                                    color:"#00ffff"

                                }


                            }


                        }


                    }


                }



            }


        }

    }

    function updateData(data,id)
    {


        flightId=id



        // 先清空

        abnormalList=[]



        // 强制刷新

        updateFlag++



        // 下一帧重新赋值

        if(data)
        {

            abnormalList=data

        }
        else
        {

            abnormalList=[]

        }


    }



}