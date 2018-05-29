<?php

class LiveViewModel extends ViewModel {
    public $viewFields = array(
        'Live'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Room'=>array('room_id','room_name','room_addr','room_desc','_on'=>'Live.room_id=Room.room_id','_type'=>'LEFT'), //Must be LEFT
        'Teacher'=>array('teacher_id','teacher_name','title_name','sex_name','detail_short','detail_info','_on'=>'Live.teacher_id=Teacher.teacher_id','_type'=>'LEFT'),
        'Lesson'=>array('lesson_id','image_id','grade_id','subject_id','lesson_order','lesson_name','lesson_desc','_on'=>'Live.lesson_id=Lesson.lesson_id','_type'=>'LEFT'),
        'Subject'=>array('subject_id','subject_name','lesson_count','_on'=>'Lesson.subject_id=Subject.subject_id','_type'=>'LEFT'),
        'Grade'=>array('grade_id','grade_type','grade_name','_on'=>'Lesson.grade_id=Grade.grade_id','_type'=>'LEFT'),
        'Image'=>array('image_id','file_fdfs'=>'image_fdfs','_on'=>'Image.image_id=Lesson.image_id'),
    );
}
?>