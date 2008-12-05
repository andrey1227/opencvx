#ifdef _MSC_VER
#pragma comment(lib, "cv.lib")
#pragma comment(lib, "cxcore.lib")
#pragma comment(lib, "cvaux.lib")
#pragma comment(lib, "highgui.lib")
#pragma warning(disable: 4996)
#endif

#include <cv.h>
#include <highgui.h>
#include <ctype.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
  CvCapture *capture = 0;
  IplImage *frame = 0;
  CvVideoWriter *vw;
  double w = 320, h = 240;
  int c, num = 0;
  CvFont font;
  //char str[64];

  // (1)�R�}���h�����ɂ���Ďw�肳�ꂽ�ԍ��̃J�����ɑ΂���L���v�`���\���̂��쐬����
  if (argc == 1 || (argc == 2 && strlen (argv[1]) == 1 && isdigit (argv[1][0])))
    capture = cvCaptureFromCAM (argc == 2 ? argv[1][0] - '0' : 0);

  // (2)�L���v�`���T�C�Y��ݒ肷��
  cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_WIDTH, w);
  cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_HEIGHT, h);

  // (3)�r�f�I���C�^�\���̂��쐬����
  printf ("Write to cap.avi. Finish with Esc.\n");
  vw = cvCreateVideoWriter ("cap.avi", CV_FOURCC ('X', 'V', 'I', 'D'), 15, cvSize ((int) w, (int) h));

  cvInitFont (&font, CV_FONT_HERSHEY_COMPLEX, 0.7, 0.7);
  cvNamedWindow ("Capture", CV_WINDOW_AUTOSIZE);

  // (4)�J��������摜���L���v�`�����C�t�@�C���ɏ����o��
  while (1) {
    frame = cvQueryFrame (capture);
    //snprintf (str, 64, "%03d[frame]", num);
    //cvPutText (frame, str, cvPoint (10, 20), &font, CV_RGB (0, 255, 100));
    cvWriteFrame (vw, frame);
    cvShowImage ("Capture", frame);
    num++;
    c = cvWaitKey (10);
    if (c == '\x1b')
      break;
  }
  // (5)�������݂��I�����C�\���̂��������
  cvReleaseVideoWriter (&vw);
  cvReleaseCapture (&capture);
  cvDestroyWindow ("Capture");

  return 0;
}
