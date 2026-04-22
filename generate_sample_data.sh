#!/bin/sh
# generate_sample_data.sh
# Creates MCASampleData1 and MCASampleData2 in current directory
# Run: sh generate_sample_data.sh

mkdir -p MCASampleData1 MCASampleData2

# ---------- MCASampleData1 : Student records ----------
gen_student() {
    idx=$1
    folder="MCASampleData1"
    cat > "$folder/student_$(printf '%03d' $idx).txt" <<EOF
Name: $2
Roll: $idx
Branch: $3
Semester: $4
CGPA: $5
City: $6
EOF
}

gen_student 1  "Alice Sharma"    "MCA" "2" "8.5"  "Kanpur"
gen_student 2  "Bob Verma"       "MCA" "2" "7.2"  "Lucknow"
gen_student 3  "Carol Singh"     "MCA" "2" "9.1"  "Delhi"
gen_student 4  "David Kumar"     "MCA" "2" "6.8"  "Agra"
gen_student 5  "Eva Gupta"       "MCA" "2" "8.0"  "Kanpur"
gen_student 6  "Frank Yadav"     "MCA" "2" "7.5"  "Varanasi"
gen_student 7  "Grace Tiwari"    "MCA" "4" "9.3"  "Kanpur"
gen_student 8  "Hari Mishra"     "MCA" "4" "5.9"  "Allahabad"
gen_student 9  "Isha Joshi"      "MCA" "4" "8.8"  "Noida"
gen_student 10 "Jay Pandey"      "MCA" "4" "7.0"  "Meerut"

echo "MCASampleData1 created with 10 student records."

# ---------- MCASampleData2 : Course records ----------
gen_course() {
    idx=$1
    folder="MCASampleData2"
    cat > "$folder/course_$(printf '%03d' $idx).txt" <<EOF
CourseCode: $2
CourseName: $3
Credits: $4
Faculty: $5
Semester: $6
EOF
}

gen_course 1 "MCA101" "Data Structures"          "4" "Dr. Siddharth Srivastava" "2"
gen_course 2 "MCA102" "Database Management"      "4" "Dr. Siddharth Srivastava" "2"
gen_course 3 "MCA103" "Operating Systems"        "3" "Prof. Anil Sharma"        "2"
gen_course 4 "MCA104" "Computer Networks"        "3" "Prof. Reena Gupta"        "4"
gen_course 5 "MCA105" "Compiler Design"          "4" "Dr. Mohit Verma"          "4"

echo "MCASampleData2 created with 5 course records."
echo ""
echo "Now compile and run:"
echo "  gcc -o dbms main.c -lm"
echo "  ./dbms"
echo ""
echo "Sample SSQL queries to try after loading:"
echo "  SELECT * FROM students"
echo "  SELECT Name,CGPA FROM students WHERE Semester=2"
echo "  SELECT * FROM students ORDER BY CGPA NUM"
echo "  SELECT * FROM students JOIN courses ON Semester=Semester TYPE INNER"
echo "  INSERT INTO students VALUES Name=Zara,Roll=11,Branch=MCA,Semester=2,CGPA=9.5,City=Mumbai"
echo "  DELETE FROM students WHERE Roll=11"
echo "  UPDATE students SET CGPA=9.0 WHERE Name=Alice Sharma"
echo "  SAVE students"
