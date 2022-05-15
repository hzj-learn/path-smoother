
#include "path_smoother.h"
#include "math_utils.h"


PathSmoother::PathSmoother(cv::Mat map_img, const std::vector<Vec2d>& cv_path) {
  map_height_ = map_img.rows;
  map_width_ = map_img.cols;

  if(map_img.channels() != 1) {
    map_img_ = map_img.clone();
    cv::cvtColor(map_img, map_img, CV_BGR2GRAY);
  } else {
    cv::cvtColor(map_img, map_img_, CV_GRAY2BGR);
  }


  //transform path from cv coordinate system to OccupancyGrid coordinate system?
  for(const auto& pt : cv_path) {
    original_path_.emplace_back(pt.x(), pt.y(), 0);
    cv::circle(map_img_, cv::Point(pt.x(), pt.y()), 1, cv::Scalar(0,255,0), 5);
  }
//  cv::imshow("original path", map_img_);
//  cv::waitKey();
}

void PathSmoother::smoothPath() {
  int iterations = 0;
  smoothed_path_ = original_path_;
  float totalWeight = wSmoothness_ + wCurvature_ ;
  
  //Todo:make sure the cycle end condition
  while (iterations < Constants::max_iterations) {
    for (int i = 1; i < smoothed_path_.size() - 1; ++i) {
      Vec2d xim1(smoothed_path_[i - 1].x(), smoothed_path_[i - 1].y());
      Vec2d xi(smoothed_path_[i].x(), smoothed_path_[i].y());
      Vec2d xip1(smoothed_path_[i + 1].x(), smoothed_path_[i + 1].y());
      Vec2d correction;

      // correction = correction - obstacleTerm(xi);

      // // std::cout<<"correction"<<correction.x()<<std::endl;
      // if (!isOnGrid(xi + correction)) { continue; }

      correction = correction - smoothnessTerm(xim1, xi, xip1);
      if (!isOnGrid(xi + correction)) { continue; }

      correction = correction - curvatureTerm(xim1, xi, xip1);
      if (!isOnGrid(xi + correction)) { continue; }

      // correction = correction - voronoiTerm(xi);
      // if (!isOnGrid(xi + correction)) { continue; }

//      Vec2d delta_change = alpha_ * correction/totalWeight;
//      std::cout <<"iterations=" << iterations <<", point index=" << i << ", delta_change=(" << delta_change.x() << ", " << delta_change.y() << ")" << std::endl;

      xi = xi + alpha_ * correction/totalWeight;
      smoothed_path_[i].set_x(xi.x());
      smoothed_path_[i].set_y(xi.y());
      Vec2d Dxi = xi - xim1;
      smoothed_path_[i - 1].set_theta(std::atan2(Dxi.y(), Dxi.x()));
    }
    iterations++;
  }

  for(const auto& po : smoothed_path_) {
    cv::circle(map_img_, cv::Point(po.x(), po.y()), 1, cv::Scalar(0,0,255), 5);
  }
  cv::imshow("smoothed path", map_img_);
  cv::waitKey();
}



Vec2d PathSmoother::curvatureTerm(Vec2d xim1, Vec2d xi, Vec2d xip1) {
  Vec2d gradient;
  // the vectors between the nodes
  Vec2d Dxi = xi - xim1;
  Vec2d Dxip1 = xip1 - xi;
  // orthogonal complements vector
  Vec2d p1, p2;

  float absDxi = Dxi.Length();
  float absDxip1 = Dxip1.Length();

  // ensure that the absolute values are not null
  if (absDxi > 0 && absDxip1 > 0) {
    float Dphi = std::acos(Clamp<float>(Dxi.InnerProd(Dxip1) / (absDxi * absDxip1), -1, 1));
    float kappa = Dphi / absDxi;

    if (kappa <= kappaMax_) {
      Vec2d zeros;
//      std::cout << "curvatureTerm is 0 because kappa(" << kappa << ") < kappamax(" << kappaMax << ")" << std::endl;
      return zeros;
    } else {
      //代入原文公式(2)与(3)之间的公式. 参考：
      // Dolgov D, Thrun S, Montemerlo M, et al. Practical search techniques in path planning for 
      //  autonomous driving[J]. Ann Arbor, 2008, 1001(48105): 18-80.
      float absDxi1Inv = 1 / absDxi;
      float PDphi_PcosDphi = -1 / std::sqrt(1 - std::pow(std::cos(Dphi), 2));
      float u = -absDxi1Inv * PDphi_PcosDphi;
      p1 = xi.ort(-xip1) / (absDxi * absDxip1);//公式(4)
      p2 = -xip1.ort(xi) / (absDxi * absDxip1);
      float s = Dphi / (absDxi * absDxi);
      Vec2d ones(1, 1);
      Vec2d ki = u * (-p1 - p2) - (s * ones);
      Vec2d kim1 = u * p2 - (s * ones);
      Vec2d kip1 = u * p1;
      gradient = wCurvature_ * (0.25 * kim1 + 0.5 * ki + 0.25 * kip1);

      if (std::isnan(gradient.x()) || std::isnan(gradient.y())) {
//        std::cout << "nan values in curvature term" << std::endl;
        Vec2d zeros;
//        std::cout << "curvatureTerm is 0 because gradient is non" << std::endl;
        return zeros;
      }
      else {
//        std::cout << "curvatureTerm is (" << gradient.x() << ", " << gradient.y() << ")" << std::endl;
        return gradient;
      }
    }
  } else {
    std::cout << "abs values not larger than 0" << std::endl;
    std::cout << absDxi << absDxip1 << std::endl;
    Vec2d zeros;
    std::cout << "curvatureTerm is 0 because abs values not larger than 0" << std::endl;
    return zeros;
  }
}

Vec2d PathSmoother::smoothnessTerm(Vec2d xim, Vec2d xi, Vec2d xip) {
  // according to paper "Practical search techniques in path planning for autonomous driving"
  return wSmoothness_ * (-4) * (xip - 2*xi + xim);
}

bool PathSmoother::isOnGrid(Vec2d vec) {
  if (vec.x() >= 0 && vec.x() < map_width_ &&
      vec.y() >= 0 && vec.y() < map_height_) {
    return true;
  }
  return false;
}
